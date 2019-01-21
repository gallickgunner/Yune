/******************************************************************************
 *  This file is part of "Yune".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Yune" is a Physically based Renderer using Bi-Directional Path Tracing.
 *  Right now the renderer only  works for devices that support OpenCL and OpenGL.
 *
 *  "Yune" is a free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  "Yune" is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#ifndef RUNTIMEEXCEPTION_H
#define RUNTIMEEXCEPTION_H

#include <string>
#include <exception>

namespace yune
{
    /** \brief A custom Exception class which stores the line number and file name where the exception occurred.
     *
     */
    class RuntimeError : public std::exception
    {
        public:
            RuntimeError(); /**< Default Constructor */

            /** \brief Overloaded Constructor.
             *
             * \param[in] msg The error message.
             */
            RuntimeError(std::string msg);

            /** \brief Overloaded Constructor.
             *
             * \param[in] msg The error message.
             * \param[in] filename The filename in which the exception was thrown.
             * \param[in] line_no The line number where the error occurred or where the exception was thrown.
             */
            RuntimeError(std::string msg, std::string filename, std::string line_no);

            ~RuntimeError();    /**< Default destructor */


            virtual const char* what() const noexcept;  /**< Definition of the virtual method of std::exception class; Returns the error message stored. */
            const char* getFileName() const noexcept;   /**< Returns the File name stored. */
            const char* getLineNo() const noexcept;     /**< Returns the Line number stored/ */

        private:
            std::string msg;       /**< The error message. */
            std::string filename;  /**< The filename in which the error occurred. */
            std::string line_no;   /**< The line number where error occured. */

    };
}
#endif // RUNTIMEEXCEPTION_H
